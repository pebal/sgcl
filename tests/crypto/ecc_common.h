//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of P-256 and P-384 share: hex in and out, random bytes of a
// fixed seed, the two curves as a list for typed tests, and OpenSSL 3 as the
// oracle (EVP_PKEY over EC: keys from their scalars, signatures made and
// checked on a raw digest, RFC 6979's deterministic nonces, ECDH, SEC 1
// points, PKCS#8, SEC 1 and SPKI DER), linked to the test program alone.
#pragma once

#include "tests/types.h"
#include "sgcl/crypto/crypto.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/x509.h>

#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace ecc_test {
    namespace crypto = sgcl::crypto;
    using bytes_t = std::vector<unsigned char>;

    inline sgcl::slice<const byte> view(const bytes_t& v) {
        return sgcl::slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size());
    }

    template<class R>
    requires requires(const R& r) { r.data(); r.size(); }
    bytes_t to_bytes(const R& r) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    inline std::string hex(const bytes_t& v) {
        static const char digits[] = "0123456789abcdef";
        std::string s;
        for (unsigned char c : v) {
            s += digits[c >> 4];
            s += digits[c & 15];
        }
        return s;
    }

    inline bytes_t unhex(const std::string& s) {
        bytes_t v;
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            throw std::runtime_error("not hex");
        };
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            v.push_back((unsigned char)(nibble(s[i]) << 4 | nibble(s[i + 1])));
        }
        return v;
    }

    inline bytes_t text(const std::string& s) {
        return bytes_t(s.begin(), s.end());
    }

    struct random_source {
        std::mt19937_64 g;

        explicit random_source(uint64_t seed) : g(seed) {}

        bytes_t bytes(size_t n) {
            bytes_t v(n);
            for (auto& b : v) {
                b = (unsigned char)g();
            }
            return v;
        }

        size_t below(size_t n) {
            return size_t(g() % n);
        }
    };

    // The two curves, for typed tests
    struct P256 {
        using curve = crypto::detail::P256;
        using public_key = crypto::p256::public_key;
        using private_key = crypto::p256::private_key;
        using ecdh_key = crypto::p256::ecdh_key;
        using hash = crypto::sha256;
        static constexpr size_t size = 32;
        static constexpr const char* group = "P-256";
        static constexpr int nid = NID_X9_62_prime256v1;
        static constexpr const char* order = "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551";
        static constexpr const char* prime = "ffffffff00000001000000000000000000000000ffffffffffffffffffffffff";
    };

    struct P384 {
        using curve = crypto::detail::P384;
        using public_key = crypto::p384::public_key;
        using private_key = crypto::p384::private_key;
        using ecdh_key = crypto::p384::ecdh_key;
        using hash = crypto::sha384;
        static constexpr size_t size = 48;
        static constexpr const char* group = "P-384";
        static constexpr int nid = NID_secp384r1;
        static constexpr const char* order = "ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973";
        static constexpr const char* prime = "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff";
    };

    using Curves = ::testing::Types<P256, P384>;

    // --- the oracle ---------------------------------------------------------

    struct Pkey {
        EVP_PKEY* p = nullptr;
        Pkey() = default;
        explicit Pkey(EVP_PKEY* k) : p(k) {}
        Pkey(const Pkey&) = delete;
        Pkey(Pkey&& o) noexcept : p(o.p) { o.p = nullptr; }
        ~Pkey() { EVP_PKEY_free(p); }
        explicit operator bool() const { return p != nullptr; }
    };

    // The public point d*G of a scalar, as OpenSSL computes it
    inline bytes_t ossl_public_of(int nid, const bytes_t& d) {
        EC_GROUP* g = EC_GROUP_new_by_curve_name(nid);
        BIGNUM* k = BN_bin2bn(d.data(), int(d.size()), nullptr);
        EC_POINT* p = EC_POINT_new(g);
        EC_POINT_mul(g, p, k, nullptr, nullptr, nullptr);
        bytes_t out(1 + 2 * ((EC_GROUP_get_degree(g) + 7) / 8));
        size_t n = EC_POINT_point2oct(g, p, POINT_CONVERSION_UNCOMPRESSED, out.data(), out.size(), nullptr);
        out.resize(n);
        EC_POINT_free(p);
        BN_free(k);
        EC_GROUP_free(g);
        return out;
    }

    // Whether OpenSSL takes the bytes as a point of the curve other than
    // the identity (EC_POINT_oct2point checks that the point is on it)
    inline bool ossl_point_valid(int nid, const bytes_t& b) {
        EC_GROUP* g = EC_GROUP_new_by_curve_name(nid);
        EC_POINT* p = EC_POINT_new(g);
        bool ok = EC_POINT_oct2point(g, p, b.data(), b.size(), nullptr) == 1 && EC_POINT_is_at_infinity(g, p) == 0 && EC_POINT_is_on_curve(g, p, nullptr) == 1;
        EC_POINT_free(p);
        EC_GROUP_free(g);
        return ok;
    }

    inline Pkey ossl_key(const char* group, const bytes_t* d, const bytes_t& pub) {
        OSSL_PARAM_BLD* bld = OSSL_PARAM_BLD_new();
        OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME, group, 0);
        BIGNUM* k = nullptr;
        if (d) {
            k = BN_bin2bn(d->data(), int(d->size()), nullptr);
            OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, k);
        }
        OSSL_PARAM_BLD_push_octet_string(bld, OSSL_PKEY_PARAM_PUB_KEY, pub.data(), pub.size());
        OSSL_PARAM* params = OSSL_PARAM_BLD_to_param(bld);
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
        EVP_PKEY* key = nullptr;
        if (EVP_PKEY_fromdata_init(ctx) != 1 || EVP_PKEY_fromdata(ctx, &key, d ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY, params) != 1) {
            key = nullptr;
        }
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(bld);
        BN_free(k);
        return Pkey(key);
    }

    // An ECDSA signature (DER) of a raw digest of any length: no digest
    // set, so OpenSSL truncates it as FIPS 186-5 has it
    inline bytes_t ossl_sign(const Pkey& key, const bytes_t& digest) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_sign_init(ctx);
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

    // RFC 6979's deterministic signature (OpenSSL 3.2 and later: the
    // nonce-type parameter), the HMAC over the digest md names
    inline bytes_t ossl_sign_deterministic(const Pkey& key, const char* md, const bytes_t& digest) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        unsigned int type = 1;
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_uint(OSSL_SIGNATURE_PARAM_NONCE_TYPE, &type),
            OSSL_PARAM_construct_utf8_string(OSSL_SIGNATURE_PARAM_DIGEST, const_cast<char*>(md), 0),
            OSSL_PARAM_construct_end()};
        bytes_t sig;
        if (EVP_PKEY_sign_init_ex(ctx, params) == 1) {
            size_t n = 0;
            EVP_PKEY_sign(ctx, nullptr, &n, digest.data(), digest.size());
            sig.resize(n);
            if (EVP_PKEY_sign(ctx, sig.data(), &n, digest.data(), digest.size()) != 1) {
                n = 0;
            }
            sig.resize(n);
        }
        EVP_PKEY_CTX_free(ctx);
        return sig;
    }

    inline bool ossl_verify(const Pkey& key, const bytes_t& digest, const bytes_t& sig) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.p, nullptr);
        EVP_PKEY_verify_init(ctx);
        bool ok = EVP_PKEY_verify(ctx, sig.data(), sig.size(), digest.data(), digest.size()) == 1;
        EVP_PKEY_CTX_free(ctx);
        return ok;
    }

    inline bytes_t ossl_derive(const Pkey& mine, const Pkey& peer) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(mine.p, nullptr);
        EVP_PKEY_derive_init(ctx);
        EVP_PKEY_derive_set_peer(ctx, peer.p);
        size_t n = 0;
        EVP_PKEY_derive(ctx, nullptr, &n);
        bytes_t out(n);
        EVP_PKEY_derive(ctx, out.data(), &n);
        out.resize(n);
        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    // The private scalar OpenSSL reads from DER (PKCS#8 or SEC 1), size
    // bytes big-endian; empty when it refuses the bytes
    inline bytes_t ossl_scalar_of_der(const bytes_t& der, size_t size) {
        const unsigned char* p = der.data();
        EVP_PKEY* key = d2i_AutoPrivateKey(nullptr, &p, long(der.size()));
        bytes_t out;
        if (key) {
            BIGNUM* d = nullptr;
            if (EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_PRIV_KEY, &d) == 1) {
                out.resize(size);
                BN_bn2binpad(d, out.data(), int(size));
            }
            BN_clear_free(d);
            EVP_PKEY_free(key);
        }
        return out;
    }

    inline bytes_t ossl_pkcs8(const Pkey& key) {
        PKCS8_PRIV_KEY_INFO* info = EVP_PKEY2PKCS8(key.p);
        unsigned char* p = nullptr;
        int n = i2d_PKCS8_PRIV_KEY_INFO(info, &p);
        bytes_t out(p, p + n);
        OPENSSL_free(p);
        PKCS8_PRIV_KEY_INFO_free(info);
        return out;
    }

    // SEC 1 ECPrivateKey: OpenSSL's "type-specific" form of an EC key
    inline bytes_t ossl_sec1(const Pkey& key) {
        unsigned char* p = nullptr;
        int n = i2d_PrivateKey(key.p, &p);
        bytes_t out(p, p + n);
        OPENSSL_free(p);
        return out;
    }

    inline bytes_t ossl_spki(const Pkey& key) {
        unsigned char* p = nullptr;
        int n = i2d_PUBKEY(key.p, &p);
        bytes_t out(p, p + n);
        OPENSSL_free(p);
        return out;
    }

    // The uncompressed point of an SPKI OpenSSL reads; empty when refused
    inline bytes_t ossl_point_of_spki(const bytes_t& der) {
        const unsigned char* p = der.data();
        EVP_PKEY* key = d2i_PUBKEY(nullptr, &p, long(der.size()));
        bytes_t out;
        if (key) {
            size_t n = 0;
            EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &n);
            out.resize(n);
            EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, out.data(), out.size(), &n);
            EVP_PKEY_free(key);
        }
        return out;
    }

    // A random scalar in [1, n - 1]
    template<class T>
    bytes_t random_scalar(random_source& r) {
        bytes_t n = unhex(T::order);
        for (;;) {
            bytes_t d = r.bytes(T::size);
            if (d < n && d != bytes_t(T::size, 0)) {
                return d;
            }
        }
    }

    template<class T>
    bool all_zero(const T& object, size_t n = sizeof(T)) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&object);
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != 0) {
                return false;
            }
        }
        return true;
    }
}
