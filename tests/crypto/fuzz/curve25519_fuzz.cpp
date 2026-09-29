//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X25519 and Ed25519 on any bytes, against OpenSSL: the input's first 32
// bytes are a private key and a seed, the next 32 a peer's public key and
// an Ed25519 public key, the next 64 a signature, the rest a message.
//
//   - X25519 of the key and the peer equals OpenSSL's, or both refuse (the
//     all-zero secret);
//   - the public key decodes or not; when it does, it writes the same
//     bytes back, and a signature of the input's bytes verifies under it
//     only if OpenSSL's verifies it too (a key of small order, the
//     identity, accepts R = identity and S = 0 everywhere); under the
//     seed's own key, a point of the prime order, it never verifies;
//   - the seed's signature of the message is OpenSSL's and verifies on
//     both sides;
//   - the whole input as DER (PKCS #8 and SubjectPublicKeyInfo of both
//     curves) is read without a crash, and what is read writes itself back.
//
// A difference aborts.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> -I/opt/homebrew/opt/openssl@3/include tests/fuzz/driver.cpp \
//       tests/crypto/fuzz/curve25519_fuzz.cpp /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib -o curve25519_fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./curve25519_fuzz <seconds> <seed files...>
#include "sgcl/crypto/ed25519.h"
#include "sgcl/crypto/x25519.h"

#include <openssl/evp.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "curve25519_fuzz: %s\n", what);
            std::abort();
        }
    }

    bool ossl_x25519(const uint8_t* priv, const uint8_t* pub, uint8_t* out) {
        EVP_PKEY* a = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, priv, 32);
        EVP_PKEY* b = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, pub, 32);
        EVP_PKEY_CTX* c = EVP_PKEY_CTX_new(a, nullptr);
        size_t n = 32;
        bool ok = EVP_PKEY_derive_init(c) == 1 && EVP_PKEY_derive_set_peer(c, b) == 1 && EVP_PKEY_derive(c, out, &n) == 1;
        EVP_PKEY_CTX_free(c);
        EVP_PKEY_free(a);
        EVP_PKEY_free(b);
        return ok;
    }

    bool ossl_sign(const uint8_t* seed, const uint8_t* msg, size_t n, uint8_t* sig) {
        EVP_PKEY* k = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, 32);
        EVP_MD_CTX* c = EVP_MD_CTX_new();
        size_t len = 64;
        static const uint8_t none = 0;
        bool ok = EVP_DigestSignInit(c, nullptr, nullptr, nullptr, k) == 1
               && EVP_DigestSign(c, sig, &len, n ? msg : &none, n) == 1;
        EVP_MD_CTX_free(c);
        EVP_PKEY_free(k);
        return ok;
    }

    bool ossl_verify(const uint8_t* pub, const uint8_t* msg, size_t n, const uint8_t* sig) {
        EVP_PKEY* k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub, 32);
        EVP_MD_CTX* c = EVP_MD_CTX_new();
        static const uint8_t none = 0;
        bool ok = EVP_DigestVerifyInit(c, nullptr, nullptr, nullptr, k) == 1
               && EVP_DigestVerify(c, sig, 64, n ? msg : &none, n) == 1;
        EVP_MD_CTX_free(c);
        EVP_PKEY_free(k);
        return ok;
    }

    template<class Private, class Public>
    void der(const uint8_t* p, size_t n) {
        auto priv = Private::from_pkcs8_der(view(p, n));
        if (priv) {
            auto back = priv->to_pkcs8_der();
            // the key read is the one written back; the version, a public
            // key [1] and attributes [0] of the input are not kept
            check(back.size() == 48 && std::memcmp(back.as_slice().data() + 16, p + 16, 32) == 0, "a PKCS #8 read is not its key");
            if (n == 48) {
                std::vector<uint8_t> same(p, p + n);
                same[4] = 0;
                check(std::memcmp(back.as_slice().data(), same.data(), 48) == 0, "a PKCS #8 read does not write itself back");
            }
        }
        auto pub = Public::from_pkix_der(view(p, n));
        if (pub) {
            auto back = pub->to_pkix_der();
            check(back.size() == n && std::memcmp(back.data(), p, n) == 0, "a SubjectPublicKeyInfo read does not write itself back");
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    der<crypto::x25519::private_key, crypto::x25519::public_key>(data, size);
    der<crypto::ed25519::private_key, crypto::ed25519::public_key>(data, size);
    if (size < 128) {
        return 0;
    }
    const uint8_t* key = data;
    const uint8_t* peer = data + 32;
    const uint8_t* sig = data + 64;
    const uint8_t* msg = data + 128;
    size_t n = size - 128;

    // X25519 against OpenSSL
    auto k = crypto::x25519::private_key::from_bytes(view(key, 32));
    auto shared = k->shared_secret(*crypto::x25519::public_key::from_bytes(view(peer, 32)));
    uint8_t expected[32];
    bool ok = ossl_x25519(key, peer, expected);
    check(ok == shared.has_value(), "x25519: one side refuses, the other does not");
    check(!ok || std::memcmp(shared->bytes().data(), expected, 32) == 0, "x25519 differs from OpenSSL");

    // a public key of any bytes, and a signature of any bytes under it
    auto pub = crypto::ed25519::public_key::from_bytes(view(peer, 32));
    if (pub) {
        check(std::memcmp(pub->bytes().data(), peer, 32) == 0, "ed25519: a decoded key writes other bytes");
        // under a key of small order (the identity: R = identity, S = 0)
        // a signature is had for nothing, and RFC 8032, Go and OpenSSL all
        // accept it: what we accept, OpenSSL must accept too
        if (pub->verify(view(msg, n), view(sig, 64))) {
            check(ossl_verify(peer, msg, n, sig), "ed25519: we accept a signature of random bytes OpenSSL refuses");
        }
    }

    // the seed's key: OpenSSL's signature, verified on both sides, and the
    // input's signature bytes are not one
    auto sk = crypto::ed25519::private_key::from_seed(view(key, 32));
    auto mine = sk->sign(view(msg, n));
    uint8_t theirs[64];
    check(ossl_sign(key, msg, n, theirs), "OpenSSL cannot sign");
    check(std::memcmp(mine.data(), theirs, 64) == 0, "ed25519 signature differs from OpenSSL");
    auto own = sk->public_key();
    check(own.verify(view(msg, n), mine), "ed25519: our signature does not verify");
    check(ossl_verify(reinterpret_cast<const uint8_t*>(own.bytes().data()), msg, n, reinterpret_cast<const uint8_t*>(mine.data())),
          "ed25519: OpenSSL does not verify our signature");
    if (std::memcmp(sig, mine.data(), 64) != 0) {
        check(!own.verify(view(msg, n), view(sig, 64)), "ed25519: random signature bytes verify under the seed's key");
    }
    return 0;
}
