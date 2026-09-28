//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of the digests, HMAC, HKDF and PBKDF2 share: hex in and
// out, random bytes of a fixed seed, and OpenSSL 3 as the oracle (EVP_Digest,
// the XOF finals of SHAKE, HMAC(), EVP_KDF's HKDF and PBKDF2), linked to
// the test program alone.
#pragma once

#include "tests/types.h"
#include "sgcl/crypto/crypto.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace crypto_test {
    namespace crypto = sgcl::crypto;
    using bytes_t = std::vector<unsigned char>;

    inline sgcl::slice<const byte> view(const unsigned char* p, size_t n) {
        return sgcl::slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    inline sgcl::slice<const byte> view(const bytes_t& v) {
        return view(v.data(), v.size());
    }

    inline sgcl::slice<const byte> view(const std::string& s) {
        return view(reinterpret_cast<const unsigned char*>(s.data()), s.size());
    }

    inline sgcl::slice<byte> out_view(bytes_t& v) {
        return sgcl::slice<byte>(reinterpret_cast<byte*>(v.data()), v.size());
    }

    inline std::string hex(const unsigned char* p, size_t n) {
        static const char digits[] = "0123456789abcdef";
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            s += digits[p[i] >> 4];
            s += digits[p[i] & 15];
        }
        return s;
    }

    inline std::string hex(const bytes_t& v) {
        return hex(v.data(), v.size());
    }

    template<class R>
    requires requires(const R& r) { r.data(); r.size(); }
    std::string hex(const R& r) {
        return hex(reinterpret_cast<const unsigned char*>(r.data()), r.size());
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

    inline bytes_t repeat(unsigned char c, size_t n) {
        return bytes_t(n, c);
    }

    inline bytes_t text(const std::string& s) {
        return bytes_t(s.begin(), s.end());
    }

    // Bytes of a generator with a fixed seed, so that a failure repeats
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

    // --- the oracle ---------------------------------------------------------

    // The OpenSSL name of each digest of the module
    template<class H> const char* ossl_name();
    template<> inline const char* ossl_name<crypto::sha1>() { return "SHA1"; }
    template<> inline const char* ossl_name<crypto::sha224>() { return "SHA224"; }
    template<> inline const char* ossl_name<crypto::sha256>() { return "SHA256"; }
    template<> inline const char* ossl_name<crypto::sha384>() { return "SHA384"; }
    template<> inline const char* ossl_name<crypto::sha512>() { return "SHA512"; }
    template<> inline const char* ossl_name<crypto::sha512_256>() { return "SHA512-256"; }
    template<> inline const char* ossl_name<crypto::sha3_224>() { return "SHA3-224"; }
    template<> inline const char* ossl_name<crypto::sha3_256>() { return "SHA3-256"; }
    template<> inline const char* ossl_name<crypto::sha3_384>() { return "SHA3-384"; }
    template<> inline const char* ossl_name<crypto::sha3_512>() { return "SHA3-512"; }
    template<> inline const char* ossl_name<crypto::shake128>() { return "SHAKE128"; }
    template<> inline const char* ossl_name<crypto::shake256>() { return "SHAKE256"; }

    // A fetched EVP_MD, kept for the program's run
    inline EVP_MD* ossl_md(const char* name) {
        EVP_MD* md = EVP_MD_fetch(nullptr, name, nullptr);
        if (!md) {
            throw std::runtime_error(std::string("OpenSSL has no ") + name);
        }
        return md;
    }

    template<class H>
    EVP_MD* ossl_md() {
        static EVP_MD* md = ossl_md(ossl_name<H>());
        return md;
    }

    template<class H>
    bytes_t ossl_digest(const unsigned char* p, size_t n) {
        unsigned char out[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        if (EVP_Digest(p, n, out, &len, ossl_md<H>(), nullptr) != 1) {
            throw std::runtime_error("EVP_Digest");
        }
        return bytes_t(out, out + len);
    }

    template<class X>
    bytes_t ossl_xof(const unsigned char* p, size_t n, size_t out_size) {
        bytes_t out(out_size);
        EVP_MD_CTX* c = EVP_MD_CTX_new();
        bool ok = EVP_DigestInit_ex(c, ossl_md<X>(), nullptr) == 1 && EVP_DigestUpdate(c, p, n) == 1
               && EVP_DigestFinalXOF(c, out.data(), out_size) == 1;
        EVP_MD_CTX_free(c);
        if (!ok) {
            throw std::runtime_error("EVP_DigestFinalXOF");
        }
        return out;
    }

    template<class H>
    bytes_t ossl_hmac(const bytes_t& key, const bytes_t& msg) {
        unsigned char out[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        static const unsigned char none = 0;
        if (!HMAC(ossl_md<H>(), key.empty() ? &none : key.data(), int(key.size()), msg.data(), msg.size(), out, &len)) {
            throw std::runtime_error("HMAC");
        }
        return bytes_t(out, out + len);
    }

    inline EVP_KDF* ossl_kdf(const char* name) {
        EVP_KDF* k = EVP_KDF_fetch(nullptr, name, nullptr);
        if (!k) {
            throw std::runtime_error(std::string("OpenSSL has no KDF ") + name);
        }
        return k;
    }

    // A pointer OpenSSL takes as given even for no bytes (a null key is
    // "missing" to its HKDF)
    inline unsigned char* nonnull(const bytes_t& v) {
        static unsigned char none = 0;
        return v.empty() ? &none : const_cast<unsigned char*>(v.data());
    }

    template<class H>
    bytes_t ossl_hkdf(const bytes_t& salt, const bytes_t& ikm, const bytes_t& info, size_t n) {
        if (n == 0) {
            return {};   // OpenSSL refuses to derive nothing; the module gives nothing
        }
        static EVP_KDF* kdf = ossl_kdf("HKDF");
        EVP_KDF_CTX* c = EVP_KDF_CTX_new(kdf);
        OSSL_PARAM params[5];
        size_t i = 0;
        params[i++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>(ossl_name<H>()), 0);
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, nonnull(ikm), ikm.size());
        if (!salt.empty()) {
            params[i++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, nonnull(salt), salt.size());
        }
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, nonnull(info), info.size());
        params[i] = OSSL_PARAM_construct_end();
        bytes_t out(n);
        bool ok = EVP_KDF_derive(c, out.data(), n, params) == 1;
        EVP_KDF_CTX_free(c);
        if (!ok) {
            throw std::runtime_error("EVP_KDF_derive HKDF");
        }
        return out;
    }

    template<class H>
    bytes_t ossl_pbkdf2(const bytes_t& password, const bytes_t& salt, uint32_t iterations, size_t n) {
        static EVP_KDF* kdf = ossl_kdf("PBKDF2");
        EVP_KDF_CTX* c = EVP_KDF_CTX_new(kdf);
        int pkcs5 = 1;   // no SP 800-132 lower bounds: RFC 8018 as it is
        OSSL_PARAM params[6] = {
            OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>(ossl_name<H>()), 0),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, nonnull(password), password.size()),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, nonnull(salt), salt.size()),
            OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
            OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS5, &pkcs5),
            OSSL_PARAM_construct_end(),
        };
        if (n == 0) {
            EVP_KDF_CTX_free(c);
            return {};
        }
        bytes_t out(n);
        bool ok = EVP_KDF_derive(c, out.data(), n, params) == 1;
        EVP_KDF_CTX_free(c);
        if (!ok) {
            throw std::runtime_error("EVP_KDF_derive PBKDF2");
        }
        return out;
    }

    // --- the module's side --------------------------------------------------

    // data into a hasher `chunk` bytes an update
    template<class H>
    void feed(H& h, const bytes_t& data, size_t chunk) {
        for (size_t i = 0; i < data.size(); i += chunk) {
            h.update(view(data.data() + i, std::min(chunk, data.size() - i)));
        }
    }

    // data into a hasher in pieces of random sizes 0..max
    template<class H>
    void feed_random(H& h, const bytes_t& data, random_source& r, size_t max) {
        size_t i = 0;
        while (i < data.size()) {
            size_t take = std::min(r.below(max + 1), data.size() - i);
            h.update(view(data.data() + i, take));
            i += take;
        }
    }

    using Digests = ::testing::Types<crypto::sha1, crypto::sha224, crypto::sha256, crypto::sha384, crypto::sha512,
                                     crypto::sha512_256, crypto::sha3_224, crypto::sha3_256, crypto::sha3_384,
                                     crypto::sha3_512>;
}
