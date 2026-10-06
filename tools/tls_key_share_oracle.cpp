// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the key shares of TLS 1.3 (sgcl/net/tls/detail/
// key_share.h): OpenSSL's own key exchanges, written out as a C++ header
// the tests include. Only in this tool, never in the library.
//
//   - X25519, P-256, P-384, P-521: a private key of random bytes (the bytes the
//     test gives its entropy), its public share as OpenSSL encodes it, a
//     peer's key pair of OpenSSL's, and the secret OpenSSL derives;
//   - X25519MLKEM768: the client's ML-KEM-768 seed d ‖ z and X25519 key
//     of random bytes, its share as the ML-KEM-768 and X25519 keys of
//     OpenSSL made of them give it (ek ‖ x25519), then OpenSSL's hybrid
//     group X25519MLKEM768 encapsulating to that share: its ciphertext
//     (the server's share) and the 64-byte secret. A share or secret in
//     another order than OpenSSL's hybrid would not agree.
//
// From the root of the tree:
//
//     clang++ -std=c++20 -O1 -I/opt/homebrew/opt/openssl@3/include tools/tls_key_share_oracle.cpp \
//         -L/opt/homebrew/opt/openssl@3/lib -lcrypto -o /tmp/tls_key_share_oracle
//     /tmp/tls_key_share_oracle > tests/net/tls_key_share_vectors.h
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using bytes = std::vector<unsigned char>;

namespace {
    [[noreturn]] void fail(const char* what) {
        std::fprintf(stderr, "tls_key_share_oracle: %s\n", what);
        ERR_print_errors_fp(stderr);
        std::exit(1);
    }

    std::string hex(const bytes& b) {
        static const char* d = "0123456789abcdef";
        std::string s;
        for (unsigned char c : b) {
            s += d[c >> 4];
            s += d[c & 15];
        }
        return s;
    }

    bytes random_bytes(size_t n) {
        bytes b(n);
        if (RAND_bytes(b.data(), int(n)) != 1) {
            fail("RAND_bytes");
        }
        return b;
    }

    bytes encoded_public(EVP_PKEY* k) {
        unsigned char* p = nullptr;
        size_t n = EVP_PKEY_get1_encoded_public_key(k, &p);
        if (n == 0) {
            fail("EVP_PKEY_get1_encoded_public_key");
        }
        bytes b(p, p + n);
        OPENSSL_free(p);
        return b;
    }

    bytes derive(EVP_PKEY* mine, EVP_PKEY* peer) {
        EVP_PKEY_CTX* c = EVP_PKEY_CTX_new_from_pkey(nullptr, mine, nullptr);
        size_t n = 0;
        if (!c || EVP_PKEY_derive_init(c) <= 0 || EVP_PKEY_derive_set_peer(c, peer) <= 0 || EVP_PKEY_derive(c, nullptr, &n) <= 0) {
            fail("EVP_PKEY_derive");
        }
        bytes s(n);
        if (EVP_PKEY_derive(c, s.data(), &n) <= 0) {
            fail("EVP_PKEY_derive");
        }
        s.resize(n);
        EVP_PKEY_CTX_free(c);
        return s;
    }

    EVP_PKEY* from_data(const char* name, int selection, OSSL_PARAM* params) {
        EVP_PKEY_CTX* c = EVP_PKEY_CTX_new_from_name(nullptr, name, nullptr);
        EVP_PKEY* k = nullptr;
        if (!c || EVP_PKEY_fromdata_init(c) <= 0 || EVP_PKEY_fromdata(c, &k, selection, params) <= 0) {
            fail(name);
        }
        EVP_PKEY_CTX_free(c);
        return k;
    }

    EVP_PKEY* x25519_of(const bytes& priv) {
        EVP_PKEY* k = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, priv.data(), priv.size());
        if (!k) {
            fail("X25519 key");
        }
        return k;
    }

    EVP_PKEY* ec_of(const char* curve, const bytes& priv) {
        BIGNUM* d = BN_bin2bn(priv.data(), int(priv.size()), nullptr);
        int nid = std::strcmp(curve, "P-256") == 0 ? NID_X9_62_prime256v1 : std::strcmp(curve, "P-384") == 0 ? NID_secp384r1 : NID_secp521r1;
        EC_GROUP* g = EC_GROUP_new_by_curve_name(nid);
        EC_POINT* q = EC_POINT_new(g);
        EC_POINT_mul(g, q, d, nullptr, nullptr, nullptr);
        unsigned char pub[133];
        size_t pn = EC_POINT_point2oct(g, q, POINT_CONVERSION_UNCOMPRESSED, pub, sizeof pub, nullptr);
        OSSL_PARAM_BLD* b = OSSL_PARAM_BLD_new();
        OSSL_PARAM_BLD_push_utf8_string(b, OSSL_PKEY_PARAM_GROUP_NAME, curve, 0);
        OSSL_PARAM_BLD_push_BN(b, OSSL_PKEY_PARAM_PRIV_KEY, d);
        OSSL_PARAM_BLD_push_octet_string(b, OSSL_PKEY_PARAM_PUB_KEY, pub, pn);
        OSSL_PARAM* p = OSSL_PARAM_BLD_to_param(b);
        EVP_PKEY* k = from_data("EC", EVP_PKEY_KEYPAIR, p);
        OSSL_PARAM_free(p);
        OSSL_PARAM_BLD_free(b);
        EC_POINT_free(q);
        EC_GROUP_free(g);
        BN_free(d);
        return k;
    }

    EVP_PKEY* generate(const char* name, const char* curve) {
        EVP_PKEY* k = curve ? EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", curve) : EVP_PKEY_Q_keygen(nullptr, nullptr, name);
        if (!k) {
            fail("keygen");
        }
        return k;
    }

    // One case of a classic group: {group, private, public, peer public, secret}
    void classic(const char* group, size_t scalar) {
        bytes priv;
        EVP_PKEY* mine;
        EVP_PKEY* peer;
        if (std::strcmp(group, "x25519") == 0) {
            priv = random_bytes(32);
            mine = x25519_of(priv);
            peer = generate("X25519", nullptr);
        } else {
            const char* curve = std::strcmp(group, "secp256r1") == 0 ? "P-256" : std::strcmp(group, "secp384r1") == 0 ? "P-384" : "P-521";
            // a scalar below n: the top byte kept small enough (P-521's
            // 66 bytes hold 521 bits, its top byte 0 or 1)
            priv = random_bytes(scalar);
            priv[0] &= scalar == 66 ? 0x01 : 0x7F;
            mine = ec_of(curve, priv);
            peer = generate(nullptr, curve);
        }
        std::printf("    {\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"},\n", group, hex(priv).c_str(), hex(encoded_public(mine)).c_str(),
            hex(encoded_public(peer)).c_str(), hex(derive(mine, peer)).c_str());
        EVP_PKEY_free(mine);
        EVP_PKEY_free(peer);
    }

    // One case of X25519MLKEM768: {seed, x25519 private, client share, server share, secret}
    void hybrid() {
        bytes seed = random_bytes(64), xpriv = random_bytes(32);
        OSSL_PARAM sp[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_ML_KEM_SEED, seed.data(), seed.size()), OSSL_PARAM_construct_end()};
        EVP_PKEY* mlkem = from_data("ML-KEM-768", EVP_PKEY_KEYPAIR, sp);
        EVP_PKEY* x = x25519_of(xpriv);
        bytes share = encoded_public(mlkem);
        bytes xpub = encoded_public(x);
        share.insert(share.end(), xpub.begin(), xpub.end());
        // OpenSSL's hybrid group, encapsulating to the share
        OSSL_PARAM hp[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, share.data(), share.size()), OSSL_PARAM_construct_end()};
        EVP_PKEY* h = from_data("X25519MLKEM768", EVP_PKEY_PUBLIC_KEY, hp);
        EVP_PKEY_CTX* c = EVP_PKEY_CTX_new_from_pkey(nullptr, h, nullptr);
        size_t ctn = 0, sn = 0;
        if (!c || EVP_PKEY_encapsulate_init(c, nullptr) <= 0 || EVP_PKEY_encapsulate(c, nullptr, &ctn, nullptr, &sn) <= 0) {
            fail("EVP_PKEY_encapsulate");
        }
        bytes ct(ctn), secret(sn);
        if (EVP_PKEY_encapsulate(c, ct.data(), &ctn, secret.data(), &sn) <= 0) {
            fail("EVP_PKEY_encapsulate");
        }
        std::printf("    {\"%s\", \"%s\",\n     \"%s\",\n     \"%s\",\n     \"%s\"},\n", hex(seed).c_str(), hex(xpriv).c_str(), hex(share).c_str(), hex(ct).c_str(), hex(secret).c_str());
        EVP_PKEY_CTX_free(c);
        EVP_PKEY_free(h);
        EVP_PKEY_free(x);
        EVP_PKEY_free(mlkem);
    }
}

int main() {
    std::printf("// Made by tools/tls_key_share_oracle.cpp (OpenSSL %s): do not edit.\n", OpenSSL_version(OPENSSL_VERSION_STRING));
    std::printf("#pragma once\n\nnamespace tls_key_share_vectors {\n");
    std::printf("    // A classic group: the private key's bytes, its share, a peer's share of OpenSSL's, the secret\n");
    std::printf("    struct Classic {\n        const char *group, *priv, *share, *peer, *secret;\n    };\n\n");
    std::printf("    inline const Classic classic[] = {\n");
    for (int i = 0; i < 4; ++i) {
        classic("x25519", 32);
        classic("secp256r1", 32);
        classic("secp384r1", 48);
        classic("secp521r1", 66);
    }
    std::printf("    };\n\n");
    std::printf("    // X25519MLKEM768: the client's seed d||z and X25519 key, its share, OpenSSL's\n");
    std::printf("    // hybrid encapsulation to it (the server's share) and the secret\n");
    std::printf("    struct Hybrid {\n        const char *seed, *x25519, *share, *server_share, *secret;\n    };\n\n");
    std::printf("    inline const Hybrid hybrid[] = {\n");
    for (int i = 0; i < 4; ++i) {
        hybrid();
    }
    std::printf("    };\n}\n");
}
