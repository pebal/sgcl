//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// RSA on any bytes. The input's first byte chooses what the rest is (mod 8):
//
//   0  an RSAPrivateKey (PKCS #1): what from_pkcs1_der takes OpenSSL must
//      take too, and it must write back to the very bytes (strict DER in,
//      canonical DER out) and sign what its public key verifies
//   1  a PKCS #8 key: what from_pkcs8_der takes OpenSSL must take, and it
//      must read back from what it writes to the same key
//   2  a SubjectPublicKeyInfo, 3  an RSAPublicKey: what is taken OpenSSL
//      takes, with the same modulus, and it writes back to the same bytes
//   4  a SHA-256 digest (32 bytes) and a PKCS #1 v1.5 signature under a
//      fixed 1024-bit key: verify_digest must agree with OpenSSL whenever
//      the signature has the key's length, and be true only for the one
//      valid input made here
//   5  the same for PSS (our salt length read from the signature, OpenSSL's
//      RSA_PSS_SALTLEN_AUTO)
//   6  an OAEP ciphertext (SHA-256, no label) under the fixed key: our
//      decryption and OpenSSL's both fail or both give the same message
//   7  a modulus and an exponent for from_modulus: what it takes OpenSSL
//      reads back from its SPKI
//
// A disagreement aborts; ASan and UBSan catch the rest.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> -I/opt/homebrew/opt/openssl@3/include tests/fuzz/driver.cpp \
//       tests/crypto/fuzz/rsa_fuzz.cpp /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib -o rsa_fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./rsa_fuzz <seconds> <seed files...>
//
// sgcl_rsa_fuzz_seeds(dir), called from a main of one line linked with this
// file instead of the driver, writes a valid input of each kind into dir.
#include "sgcl/crypto/crypto.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    using bytes_t = std::vector<unsigned char>;
    using crypto::hash_id;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    template<class R>
    bytes_t to_bytes(const R& r) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "rsa_fuzz: %s\n", what);
            std::abort();
        }
    }

    bytes_t unhex(const char* s) {
        bytes_t v;
        auto nib = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        for (; s[0] && s[1]; s += 2) {
            v.push_back((unsigned char)(nib(s[0]) << 4 | nib(s[1])));
        }
        return v;
    }

    // The fixed 1024-bit key (OpenSSL's, PKCS #1)
    const char* const key_hex =
        "3082025b02010002818100acfbec6474ecac0aa619c86ddefea6785560ba44220d3e7ffebd0ebdb77cef696076588addc422392abe473a2f"
        "0955db894ff3f52e520ddb422d377237b7f39ca379462e7902fe335854df1b3c3f15fd84133357215d31bedfaeef0e877ae3219e1812c6c1"
        "5bf40a06fcc54ff36087a5fbaf3f202f2adcfd185225d20cc5c1b9020301000102818065f954cf9b85f19d8d7161903386d6c7d53ce2514b"
        "e996d0897116018ceb5f8f35484ec5fe19a33b24116b97c726afb06d6fee96b8a9b27570bb8b160ff6a6ead5c8a66a069ea021b73aa9ca14"
        "0c2671e70ef0437df1dd740d8a023b57d075777ff8c5d75aa2e0ba8173cb41005e20e34db28bf252b3903825c22c83554f0e09024100d9f2"
        "9db543d907a989e345651e805eb3469e9a31cd6999f3f9e3c70cc0db28567b23bd4f403d24bf96ed2b72a77bdfb9f54c8ad631111ae44e4d"
        "af2b5d6a2c1f024100cb2f9d26bae42ee999ab635ef4805f62a42f110333800f9c11d84057cc4c1fb9b32aca389e24571b74c6e1250fc907"
        "7d746f99f34d9de822363c5fd1f815d727024001fb66dbc326321fa49560882ee2d3f9a566c3d8381a01c06a415c0a0da6f092f8c3b67106"
        "10884905c25a66cc871fddbd115921e1885a4a413c0b9fc555b5d902403f359cca03b220f6031c5259a3ee9bbdf01dc4491ea86432c54a65"
        "19eb30735e2de2cdee37244f04f79b37477455b1c2d18a36767e5af5704f3711081d098e59024033915991112e940d138cd381549c4aa59e"
        "5be7ddf3732dc65e88d0b7bd1764d2a12c24e3c83aabc4e3ad7057ca50c1d3e7fcfe2202d2d3be01fc5a94b290041c";

    const crypto::rsa::private_key& key() {
        static const crypto::rsa::private_key k = [] {
            bytes_t der = unhex(key_hex);
            return std::move(*crypto::rsa::private_key::from_pkcs1_der(view(der.data(), der.size())));
        }();
        return k;
    }

    EVP_PKEY* ossl_key() {
        static EVP_PKEY* k = [] {
            bytes_t der = unhex(key_hex);
            const unsigned char* p = der.data();
            return d2i_PrivateKey(EVP_PKEY_RSA, nullptr, &p, long(der.size()));
        }();
        return k;
    }

    const bytes_t& digest() {
        static const bytes_t d = [] {
            bytes_t v(32);
            for (size_t i = 0; i < 32; ++i) {
                v[i] = (unsigned char)(i * 7 + 3);
            }
            return v;
        }();
        return d;
    }

    // The one valid input of each verification: the digest and its
    // signature (v1.5 is deterministic; the PSS one made once, kept)
    const bytes_t& valid_v15() {
        static const bytes_t v = [] {
            bytes_t in = digest();
            auto s = key().sign_digest(hash_id::sha256, view(digest().data(), 32));
            in.insert(in.end(), reinterpret_cast<const unsigned char*>(s.data()), reinterpret_cast<const unsigned char*>(s.data()) + s.size());
            return in;
        }();
        return v;
    }

    const bytes_t& valid_pss() {
        static const bytes_t v = [] {
            bytes_t in = digest();
            auto s = key().sign_digest_pss(hash_id::sha256, view(digest().data(), 32));
            in.insert(in.end(), reinterpret_cast<const unsigned char*>(s.data()), reinterpret_cast<const unsigned char*>(s.data()) + s.size());
            return in;
        }();
        return v;
    }

    bytes_t ossl_modulus(EVP_PKEY* k) {
        BIGNUM* n = nullptr;
        EVP_PKEY_get_bn_param(k, OSSL_PKEY_PARAM_RSA_N, &n);
        bytes_t out(size_t(BN_num_bytes(n)));
        BN_bn2bin(n, out.data());
        BN_free(n);
        return out;
    }

    void private_pkcs1(const uint8_t* p, size_t n) {
        auto k = crypto::rsa::private_key::from_pkcs1_der(view(p, n));
        if (!k) {
            return;
        }
        const unsigned char* q = p;
        EVP_PKEY* o = d2i_PrivateKey(EVP_PKEY_RSA, nullptr, &q, long(n));
        check(o != nullptr, "from_pkcs1_der took what OpenSSL refuses");
        EVP_PKEY_free(o);
        bytes_t back = to_bytes(k->to_pkcs1_der());
        check(back == bytes_t(p, p + n), "a PKCS#1 key does not write back to its bytes");
        auto s = k->sign_digest(hash_id::sha256, view(digest().data(), 32));
        check(k->public_key().verify_digest(hash_id::sha256, view(digest().data(), 32), s.as_slice()), "a key's signature does not verify");
    }

    void private_pkcs8(const uint8_t* p, size_t n) {
        auto k = crypto::rsa::private_key::from_pkcs8_der(view(p, n));
        if (!k) {
            return;
        }
        const unsigned char* q = p;
        EVP_PKEY* o = d2i_AutoPrivateKey(nullptr, &q, long(n));
        check(o != nullptr, "from_pkcs8_der took what OpenSSL refuses");
        EVP_PKEY_free(o);
        auto again = crypto::rsa::private_key::from_pkcs8_der(k->to_pkcs8_der().as_slice());
        check(again.has_value() && to_bytes(again->to_pkcs1_der()) == to_bytes(k->to_pkcs1_der()), "a PKCS#8 key does not read back");
    }

    void public_key(const uint8_t* p, size_t n, bool spki) {
        auto k = spki ? crypto::rsa::public_key::from_pkix_der(view(p, n)) : crypto::rsa::public_key::from_pkcs1_der(view(p, n));
        if (!k) {
            return;
        }
        const unsigned char* q = p;
        EVP_PKEY* o = spki ? d2i_PUBKEY(nullptr, &q, long(n)) : d2i_PublicKey(EVP_PKEY_RSA, nullptr, &q, long(n));
        check(o != nullptr, "a public key taken that OpenSSL refuses");
        check(ossl_modulus(o) == to_bytes(k->modulus()), "a public key's modulus differs from OpenSSL's");
        EVP_PKEY_free(o);
        bytes_t back = spki ? to_bytes(k->to_pkix_der()) : to_bytes(k->to_pkcs1_der());
        check(back == bytes_t(p, p + n), "a public key does not write back to its bytes");
    }

    void verify(const uint8_t* p, size_t n, bool pss) {
        if (n < 32) {
            return;
        }
        auto pub = key().public_key();
        bool ours = pss ? pub.verify_digest_pss(hash_id::sha256, view(p, 32), view(p + 32, n - 32)) : pub.verify_digest(hash_id::sha256, view(p, 32), view(p + 32, n - 32));
        const bytes_t& valid = pss ? valid_pss() : valid_v15();
        bool is_valid = n == valid.size() && std::memcmp(p, valid.data(), n) == 0;
        if (!pss) {
            check(ours == is_valid, ours ? "verify_digest took a forgery" : "verify_digest refused the valid signature");
        } else if (is_valid) {
            check(ours, "verify_digest_pss refused the valid signature");
        }
        if (n - 32 != pub.size()) {
            check(!ours, "a signature of another length verified");
            return;
        }
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(ossl_key(), nullptr);
        EVP_PKEY_verify_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, pss ? RSA_PKCS1_PSS_PADDING : RSA_PKCS1_PADDING);
        EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256());
        if (pss) {
            EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, RSA_PSS_SALTLEN_AUTO);
            EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());
        }
        bool ossl = EVP_PKEY_verify(ctx, p + 32, n - 32, p, 32) == 1;
        EVP_PKEY_CTX_free(ctx);
        ERR_clear_error();
        check(ossl == ours, pss ? "verify_digest_pss disagrees with OpenSSL" : "verify_digest disagrees with OpenSSL");
    }

    void decrypt(const uint8_t* p, size_t n) {
        auto m = key().decrypt_oaep(hash_id::sha256, view(p, n));
        if (n != key().size()) {
            check(!m.has_value(), "a ciphertext of another length decrypted");
            return;
        }
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(ossl_key(), nullptr);
        EVP_PKEY_decrypt_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
        EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256());
        EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());
        bytes_t out(n);
        size_t len = out.size();
        bool ok = EVP_PKEY_decrypt(ctx, out.data(), &len, p, n) == 1;
        EVP_PKEY_CTX_free(ctx);
        ERR_clear_error();
        check(ok == m.has_value(), "decrypt_oaep disagrees with OpenSSL");
        if (ok) {
            out.resize(len);
            check(out == to_bytes(*m), "decrypt_oaep gives another message than OpenSSL");
        }
    }

    void modulus(const uint8_t* p, size_t n) {
        if (n < 4) {
            return;
        }
        uint64_t e = uint64_t(p[0]) << 24 | uint64_t(p[1]) << 16 | uint64_t(p[2]) << 8 | p[3];
        auto k = crypto::rsa::public_key::from_modulus(view(p + 4, n - 4), e);
        if (k) {
            bytes_t spki = to_bytes(k->to_pkix_der());
            const unsigned char* q = spki.data();
            EVP_PKEY* o = d2i_PUBKEY(nullptr, &q, long(spki.size()));
            check(o != nullptr, "OpenSSL refuses the SPKI of a modulus taken");
            EVP_PKEY_free(o);
            check(k->exponent() == e, "the exponent changed");
        }
    }

    void write_seed(const std::string& dir, unsigned what, const bytes_t& body) {
        bytes_t in = {static_cast<unsigned char>(what)};
        in.insert(in.end(), body.begin(), body.end());
        std::ofstream(dir + "/seed-rsa-" + std::to_string(what) + ".bin", std::ios::binary)
            .write(reinterpret_cast<const char*>(in.data()), std::streamsize(in.size()));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const uint8_t* p = data + 1;
    size_t n = size - 1;
    switch (data[0] % 8) {
        case 0: private_pkcs1(p, n); break;
        case 1: private_pkcs8(p, n); break;
        case 2: public_key(p, n, true); break;
        case 3: public_key(p, n, false); break;
        case 4: verify(p, n, false); break;
        case 5: verify(p, n, true); break;
        case 6: decrypt(p, n); break;
        case 7: modulus(p, n); break;
    }
    return 0;
}

// The seeds (see the top of the file)
extern "C" int sgcl_rsa_fuzz_seeds(const char* dir) {
    write_seed(dir, 0, to_bytes(key().to_pkcs1_der()));
    write_seed(dir, 1, to_bytes(key().to_pkcs8_der()));
    write_seed(dir, 2, to_bytes(key().public_key().to_pkix_der()));
    write_seed(dir, 3, to_bytes(key().public_key().to_pkcs1_der()));
    write_seed(dir, 4, valid_v15());
    write_seed(dir, 5, valid_pss());
    bytes_t msg = {'h', 'i'};
    write_seed(dir, 6, to_bytes(key().public_key().encrypt_oaep(hash_id::sha256, view(msg.data(), msg.size()))));
    bytes_t mod = {0x00, 0x01, 0x00, 0x01};
    auto nb = key().public_key().modulus();
    mod.insert(mod.end(), reinterpret_cast<const unsigned char*>(nb.data()), reinterpret_cast<const unsigned char*>(nb.data()) + nb.size());
    write_seed(dir, 7, mod);
    return 0;
}
